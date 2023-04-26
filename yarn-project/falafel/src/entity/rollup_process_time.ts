import { Column, Entity, PrimaryGeneratedColumn } from 'typeorm';

@Entity({ name: 'rollup_process_time' })
export class RollupProcessTimeDao {
  public constructor(init?: RollupProcessTimeDao) {
    Object.assign(this, init);
  }

  @PrimaryGeneratedColumn('increment')
  public id?: number;

  @Column({ nullable: true })
  public rollupId?: number;

  @Column({ nullable: true })
  public rootRollupHash?: string;

  @Column()
  public innerRollupCount!: number;

  @Column()
  public started?: Date;

  @Column({ nullable: true })
  public innerCompleted?: Date;

  @Column({ nullable: true })
  public outerCompleted?: Date;
}
